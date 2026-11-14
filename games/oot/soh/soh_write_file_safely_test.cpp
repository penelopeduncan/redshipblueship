/**
 * @file soh_write_file_safely_test.cpp
 * ROM-free, display-free lock for #1467 (item 2): SaveManager::WriteFileSafely
 * and its Presets and Plandomizer callers write via a temp file and rename
 * instead of truncating the target in place. CTest label "redship", row
 * WriteFileSafelyWriters in CMake/SingleExecutable.cmake, dispatch
 * "write-file-safely-writers" in src/common/test_runner.cpp.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "SaveManager.h"
#include "test_scratch.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

bool Presets_WriteFile(const std::filesystem::path& target, const nlohmann::json& values);
bool Plandomizer_WriteSpoilerFile(const std::filesystem::path& target, const nlohmann::json& spoilerSave);

namespace {

#define WFS_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            failures++;                               \
        }                                             \
    } while (0)

bool WfsReadBytes(const std::filesystem::path& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

bool WfsWriteBytes(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    out.flush();
    return out.good();
}

using WfsWriteFn = bool (*)(const std::filesystem::path& target, const nlohmann::json& payload,
                            const std::string& rawContent, std::string* expectedBytes);

struct WfsWriterCase {
    const char* name;
    WfsWriteFn write;
};

bool WfsCallSaveManager(const std::filesystem::path& target, const nlohmann::json& /*payload*/,
                        const std::string& rawContent, std::string* expectedBytes) {
    *expectedBytes = rawContent;
    return SaveManager::WriteFileSafely(target, target.string() + ".tmp", rawContent);
}

bool WfsCallPresets(const std::filesystem::path& target, const nlohmann::json& payload,
                    const std::string& /*rawContent*/, std::string* expectedBytes) {
    *expectedBytes = payload.dump(4);
    return Presets_WriteFile(target, payload);
}

bool WfsCallPlandomizer(const std::filesystem::path& target, const nlohmann::json& payload,
                        const std::string& /*rawContent*/, std::string* expectedBytes) {
    *expectedBytes = payload.dump(4);
    return Plandomizer_WriteSpoilerFile(target, payload);
}

void WfsRunLegs(const WfsWriterCase& writer, const std::filesystem::path& baseDir, int& failures) {
    const std::filesystem::path writerDir = baseDir / writer.name;
    std::error_code ec;
    std::filesystem::create_directories(writerDir, ec);
    WFS_ASSERT(!ec, "%s: create_directories failed: %s", writer.name, ec.message().c_str());

    // Leg a: fresh write (target does not exist).
    {
        const std::filesystem::path target = writerDir / "leg_a.json";
        const std::filesystem::path temp = target.string() + ".tmp";
        const nlohmann::json payload = { { "leg", "a" }, { "fresh", true } };
        std::string expected;
        const bool ok = writer.write(target, payload, "FRESH", &expected);
        std::string actual;
        WFS_ASSERT(ok, "%s leg a: expected fresh write to return true", writer.name);
        WFS_ASSERT(WfsReadBytes(target, &actual) && actual == expected,
                   "%s leg a: target bytes do not match expected content", writer.name);
        WFS_ASSERT(!std::filesystem::exists(temp, ec), "%s leg a: temp file still exists after write", writer.name);
    }

    // Leg b: overwrite (target holds OLD -> writes NEW).
    {
        const std::filesystem::path target = writerDir / "leg_b.json";
        const std::filesystem::path temp = target.string() + ".tmp";
        WFS_ASSERT(WfsWriteBytes(target, "OLD"), "%s leg b: failed to seed target with OLD", writer.name);
        const nlohmann::json payload = { { "leg", "b" }, { "state", "NEW" } };
        std::string expected;
        const bool ok = writer.write(target, payload, "NEW", &expected);
        std::string actual;
        WFS_ASSERT(ok, "%s leg b: expected overwrite to return true", writer.name);
        WFS_ASSERT(WfsReadBytes(target, &actual) && actual == expected,
                   "%s leg b: target bytes do not match NEW content", writer.name);
        WFS_ASSERT(!std::filesystem::exists(temp, ec), "%s leg b: temp file still exists after overwrite", writer.name);
    }

    // Leg c: stale temp (regular file already sits at temp holding junk).
    {
        const std::filesystem::path target = writerDir / "leg_c.json";
        const std::filesystem::path temp = target.string() + ".tmp";
        WFS_ASSERT(WfsWriteBytes(temp, "stale-temp-junk"), "%s leg c: failed to seed stale temp file", writer.name);
        const nlohmann::json payload = { { "leg", "c" }, { "staleTemp", "cleared" } };
        std::string expected;
        const bool ok = writer.write(target, payload, "STALE_REPLACED", &expected);
        std::string actual;
        WFS_ASSERT(ok, "%s leg c: expected write over stale temp to return true", writer.name);
        WFS_ASSERT(WfsReadBytes(target, &actual) && actual == expected,
                   "%s leg c: target bytes do not match expected content", writer.name);
        WFS_ASSERT(!std::filesystem::exists(temp, ec), "%s leg c: temp file still exists after write", writer.name);
    }

    // Leg d: failed write leaves target intact (non-empty directory at temp path).
    {
        const std::filesystem::path target = writerDir / "leg_d.json";
        const std::filesystem::path temp = target.string() + ".tmp";
        WFS_ASSERT(WfsWriteBytes(target, "KEEP"), "%s leg d: failed to seed target with KEEP", writer.name);
        std::filesystem::create_directories(temp, ec);
        WFS_ASSERT(!ec, "%s leg d: failed to create directory at temp path: %s", writer.name, ec.message().c_str());
        WFS_ASSERT(WfsWriteBytes(temp / "blocker", "non-empty"),
                   "%s leg d: failed to create blocker file inside temp directory", writer.name);
        const nlohmann::json payload = { { "leg", "d" }, { "shouldNotLand", true } };
        std::string unusedExpected;
        const bool ok = writer.write(target, payload, "OVERWRITE_SHOULD_FAIL", &unusedExpected);
        std::string actual;
        WFS_ASSERT(!ok, "%s leg d: expected failed write to return false", writer.name);
        WFS_ASSERT(WfsReadBytes(target, &actual) && actual == "KEEP",
                   "%s leg d: target was modified or lost on failed write", writer.name);
    }
}

} // namespace

extern "C" int Soh_WriteFileSafelyWriters_RunHeadless(void) {
    printf("[TEST] write-file-safely-writers: SaveManager::WriteFileSafely, Presets_WriteFile and "
           "Plandomizer_WriteSpoilerFile write atomically and leave the target intact on failure (#1467)\n");
    int failures = 0;

    const std::filesystem::path dir = rsbs::test::ScratchDir("rsbs_soh_write_file_safely_test");
    const std::vector<WfsWriterCase> writers = {
        { "save_manager", WfsCallSaveManager },
        { "presets", WfsCallPresets },
        { "plandomizer", WfsCallPlandomizer },
    };
    for (const WfsWriterCase& writer : writers) {
        WfsRunLegs(writer, dir, failures);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    printf("[TEST] write-file-safely-writers: %s (%d failure(s))\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures;
}

#endif // RSBS_SINGLE_EXECUTABLE
