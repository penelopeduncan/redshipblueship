/**
 * @file test_home.cpp
 * @brief A private working directory for each CTest row that asks for one (#1692)
 *
 * See test_home.h for the rationale and the order of the checks.
 */

#include "test_home.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

const char* TestHomeEnv(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

#ifndef __APPLE__
// This executable's own directory, from the same source as
// Ship::Context::GetAppBundlePath (GetModuleFileNameW on Windows,
// /proc/self/exe on Linux): the directory a row in a home finds its archives in.
// Empty when it cannot be resolved.
std::filesystem::path TestHomeExeDir() {
#ifdef _WIN32
    std::wstring buf(32768, L'\0');
    const DWORD len = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
    if (len == 0 || len >= buf.size()) {
        return {};
    }
    buf.resize(len);
    return std::filesystem::path(buf).parent_path();
#else
    std::error_code ec;
    const std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return exe.parent_path();
#endif
}
#endif

bool TestHomeSameDir(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code ec;
    const bool same = std::filesystem::equivalent(a, b, ec);
    return !ec && same;
}

} // namespace

int TestHome_EnterIfAsked(const char* mode) {
    const char* homeEnv = TestHomeEnv("RSBS_TEST_HOME");
    if (homeEnv == nullptr) {
        return 0;
    }
    const char* tag = mode != nullptr ? mode : "--test";
    const char* fromEnv = TestHomeEnv("RSBS_TEST_HOME_FROM");
    if (fromEnv == nullptr) {
        printf("[HOME] FAIL: RSBS_TEST_HOME set without RSBS_TEST_HOME_FROM (%s)\n", tag);
        fflush(stdout);
        return 1;
    }
    const std::filesystem::path home(homeEnv);
    const std::filesystem::path from(fromEnv);

    std::error_code ec;
    const std::filesystem::path cwd = std::filesystem::current_path(ec);
    if (ec || !TestHomeSameDir(cwd, from)) {
        printf("[HOME] kept %s: started outside %s (%s)\n", cwd.string().c_str(), from.string().c_str(), tag);
        fflush(stdout);
        return 0;
    }

#ifndef __APPLE__
    const std::filesystem::path exeDir = TestHomeExeDir();
    if (exeDir.empty() || !TestHomeSameDir(exeDir, from)) {
        printf("[HOME] FAIL: exe dir %s is not %s, so a row in %s would not find the archives staged beside "
               "the executable (%s)\n",
               exeDir.empty() ? "(unresolved)" : exeDir.string().c_str(), from.string().c_str(), home.string().c_str(),
               tag);
        fflush(stdout);
        return 1;
    }
#endif

    std::filesystem::create_directories(home, ec);
    if (ec || !std::filesystem::is_directory(home)) {
        printf("[HOME] FAIL: could not create %s: %s (%s)\n", home.string().c_str(), ec.message().c_str(), tag);
        fflush(stdout);
        return 1;
    }

    const char* config = "none";
    const std::filesystem::path seed = from / "shipofharkinian.json";
    const std::filesystem::path seeded = home / "shipofharkinian.json";
    if (std::filesystem::exists(seeded, ec)) {
        config = "kept";
    } else if (std::filesystem::exists(seed, ec)) {
        ec.clear();
        std::filesystem::copy_file(seed, seeded, ec);
        if (ec || !std::filesystem::is_regular_file(seeded)) {
            printf("[HOME] FAIL: could not seed %s from %s: %s (%s)\n", seeded.string().c_str(), seed.string().c_str(),
                   ec ? ec.message().c_str() : "not a regular file", tag);
            fflush(stdout);
            return 1;
        }
        config = "seeded";
    }

    ec.clear();
    std::filesystem::current_path(home, ec);
    if (ec) {
        printf("[HOME] FAIL: could not enter %s: %s (%s)\n", home.string().c_str(), ec.message().c_str(), tag);
        fflush(stdout);
        return 1;
    }

    const std::string absHome = std::filesystem::absolute(home, ec).string();
#ifdef _WIN32
    _putenv_s("SHIP_HOME", absHome.c_str());
#else
    setenv("SHIP_HOME", absHome.c_str(), 1);
#endif
    printf("[HOME] %s config=%s (%s)\n", absHome.c_str(), config, tag);
    fflush(stdout);
    return 0;
}

std::string TestHome_RowOutputPath(const char* name) {
    const char* dir = TestHomeEnv("RSBS_ROW_OUTPUT_DIR");
    if (dir == nullptr) {
        return std::string(name);
    }
    return (std::filesystem::path(dir) / name).string();
}
