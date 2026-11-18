/**
 * Row-home probes (#1692), driven by CMake/CheckRowHome.cmake (the RowHomeIsolation row).
 *
 * A rando-label CTest row runs in its own home, `<build>/rsbs-rows/<row>`, which
 * TestHome_EnterIfAsked (src/common/test_home.h) makes the process's working
 * directory before any row code runs. These two dispatches let a script prove
 * that from outside, ROM-free and display-free:
 *
 *   row-home-probe       reports where it is (working directory, app directory,
 *                        whether either is RSBS_TEST_HOME) into the file named by
 *                        RSBS_ROW_HOME_REPORT, then writes one small file under
 *                        each name the real writers use, resolved exactly as they
 *                        resolve it (Ship::Context::GetPathRelativeToAppDirectory).
 *                        With RSBS_ROW_HOME_BARRIER set it first waits (up to 30 s)
 *                        until RSBS_ROW_HOME_BARRIER_COUNT probes have arrived, so
 *                        the probes of a pair really write at the same time.
 *   row-home-probe-pair  starts two row-home-probe children AT ONCE in the same
 *                        working directory with two different homes, and requires
 *                        both to exit 0. The children's reports and files are the
 *                        script's to check.
 *
 * The probe brings up no Ship::Context: the static path helpers it calls read only
 * the environment and the working directory, which is all a writer's path depends
 * on in a portable build. Without RSBS_ROW_HOME_REPORT (or RSBS_ROW_HOME_PAIR_DIR)
 * both dispatches skip, so `--test all` never writes anything for them.
 *
 * Included at FILE SCOPE by test_runner.cpp (compiled as C++).
 */

#include "test_scratch.h"

#include <ship/Context.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {

// The names the real writers resolve against the app directory (spoiler_log.cpp
// and SeedPackageSingleExe.cpp's Randomizer/, MM's randomizer-mm/, Context's logs/,
// Gui's imgui.ini, the save managers' Save/ and saves/). One small file under each.
const char* const kRhpWrites[] = {
    "Randomizer/row-home-probe.json", "randomizer-mm/row-home-probe.json", "logs/row-home-probe.log", "imgui.ini",
    "Save/row-home-probe.sav",        "saves/row-home-probe.sav",
};

const char* RhpEnv(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

bool RhpSame(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    const bool same = std::filesystem::equivalent(a, b, ec);
    return !ec && same;
}

// Waits until @p count files are in @p dir, up to 30 s. Arrives first.
bool RhpBarrier(const std::filesystem::path& dir, const std::string& id, int count) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    {
        std::ofstream mark(dir / id, std::ios::binary | std::ios::trunc);
        mark << id << '\n';
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline) {
        int seen = 0;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            (void)entry;
            seen++;
        }
        if (seen >= count) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

} // namespace

static TestResult Test_RowHomeProbe(void) {
    const char* report = RhpEnv("RSBS_ROW_HOME_REPORT");
    if (report == nullptr) {
        printf("[TEST] SKIP: row-home-probe runs only under CMake/CheckRowHome.cmake (RSBS_ROW_HOME_REPORT unset)\n");
        return TEST_SKIP;
    }
    const char* idEnv = RhpEnv("RSBS_ROW_HOME_ID");
    const std::string id = idEnv != nullptr ? idEnv : "0";
    const char* home = RhpEnv("RSBS_TEST_HOME");

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    const std::string appDir = Ship::Context::GetAppDirectoryPath();
    const bool entered = home != nullptr && RhpSame(cwd, home);
    const bool appIsHome = home != nullptr && RhpSame(appDir, home);

    std::string barrier = "none";
    if (const char* barrierDir = RhpEnv("RSBS_ROW_HOME_BARRIER")) {
        const char* countEnv = RhpEnv("RSBS_ROW_HOME_BARRIER_COUNT");
        const int count = countEnv != nullptr ? std::atoi(countEnv) : 2;
        barrier = RhpBarrier(barrierDir, id, count) ? "ok" : "timeout";
    }

    std::vector<std::string> wrote;
    int writeFailures = 0;
    for (const char* rel : kRhpWrites) {
        const std::filesystem::path target(Ship::Context::GetPathRelativeToAppDirectory(rel));
        std::filesystem::create_directories(target.parent_path(), ec);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        out << "row-home-probe id=" << id << '\n';
        out.close();
        if (!out) {
            writeFailures++;
        }
        wrote.push_back(std::filesystem::absolute(target, ec).string());
    }

    std::ofstream r(report, std::ios::binary | std::ios::trunc);
    r << "id=" << id << '\n';
    r << "home=" << (home != nullptr ? home : "") << '\n';
    r << "cwd=" << cwd.string() << '\n';
    r << "app_dir=" << std::filesystem::absolute(appDir, ec).string() << '\n';
    r << "entered=" << (entered ? 1 : 0) << '\n';
    r << "app_dir_is_home=" << (appIsHome ? 1 : 0) << '\n';
    r << "barrier=" << barrier << '\n';
    for (const std::string& w : wrote) {
        r << "wrote=" << w << '\n';
    }
    r << "write_failures=" << writeFailures << '\n';
    r << "exit=" << ((barrier == "timeout" || writeFailures != 0) ? "fail" : "pass") << '\n';
    r.close();

    printf("[TEST] row-home-probe id=%s entered=%d app_dir_is_home=%d barrier=%s write_failures=%d\n", id.c_str(),
           entered ? 1 : 0, appIsHome ? 1 : 0, barrier.c_str(), writeFailures);
    if (!r) {
        printf("[TEST] FAIL: could not write the report %s\n", report);
        return TEST_FAIL;
    }
    return (barrier == "timeout" || writeFailures != 0) ? TEST_FAIL : TEST_PASS;
}

static TestResult Test_RowHomeProbePair(void) {
    const char* dir = RhpEnv("RSBS_ROW_HOME_PAIR_DIR");
    const char* from = RhpEnv("RSBS_ROW_HOME_PAIR_FROM");
    if (dir == nullptr || from == nullptr) {
        printf("[TEST] SKIP: row-home-probe-pair runs only under CMake/CheckRowHome.cmake "
               "(RSBS_ROW_HOME_PAIR_DIR / RSBS_ROW_HOME_PAIR_FROM unset)\n");
        return TEST_SKIP;
    }
    const std::filesystem::path out(dir);
    const std::filesystem::path fromDir(from);
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    std::filesystem::remove_all(out / "barrier", ec);

    std::vector<rsbs::test::ChildSpec> children;
    for (const char* n : { "2", "3" }) {
        rsbs::test::ChildSpec child;
        child.cwd = fromDir;
        child.log = out / (std::string("child") + n + ".log");
        child.env = {
            { "RSBS_TEST_HOME", (fromDir / "rsbs-rows" / (std::string("H") + n)).string() },
            { "RSBS_TEST_HOME_FROM", fromDir.string() },
            { "RSBS_ROW_HOME_REPORT", (out / (std::string("R") + n + ".txt")).string() },
            { "RSBS_ROW_HOME_ID", n },
            { "RSBS_ROW_HOME_BARRIER", (out / "barrier").string() },
            { "RSBS_ROW_HOME_BARRIER_COUNT", "2" },
        };
        children.push_back(child);
    }
    const std::vector<int> codes = rsbs::test::RunRowWithEnvsConcurrently("row-home-probe", children);
    int failures = 0;
    for (size_t i = 0; i < codes.size(); i++) {
        const bool ok = codes[i] == 0;
        printf("[TEST] %s: row-home-probe child %zu (log %s) exited %d\n", ok ? "ok" : "FAIL", i + 2,
               children[i].log.string().c_str(), codes[i]);
        if (!ok) {
            failures++;
        }
    }
    printf("[TEST] %s: row-home-probe-pair, %d failure(s)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? TEST_PASS : TEST_FAIL;
}
