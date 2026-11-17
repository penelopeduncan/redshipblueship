/**
 * @file test_oot_hidden_not_hintable.c
 * @brief ROM-free regression check for #1667 (`oot-hidden-location-not-hintable`).
 *
 * Included at file scope by `src/common/test_runner.cpp` (compiled as C++).
 *
 * Upstream SoH #6146 (`7006ef740`) keeps a hidden placement (one made with
 * `PlaceItemInLocation(..., true)`: the unshuffled Master Sword, the vanilla
 * songs under Shuffle Songs Off, and the rest of `item_pool.cpp`'s hidden
 * placements) out of hints and out of the spoiler log's playthrough:
 *
 *  1. behaviour leg: `Rando::ItemLocation::IsHintable()` answers
 *     `isHintable && !hidden`, read through the
 *     `OoT_ItemLocation_TestHintableAnswer` bridge in `item_location.cpp`;
 *  2. source leg: `WritePlaythrough` in `3drando/spoiler_log.cpp` writes a
 *     sphere's location only inside `if (!ctx->GetItemLocation(key)->IsHidden())`,
 *     and has no unguarded `WriteLocation` call.
 */

#include "../test_runner.h"

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

extern "C" int OoT_ItemLocation_TestHintableAnswer(int hintable, int hidden);

namespace {

std::string OhnhReadSource(const char* relPath) {
#ifdef RSBS_SOURCE_DIR
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + relPath, std::ios::binary);
    if (!in.good()) {
        return std::string();
    }
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
#else
    (void)relPath;
    return std::string();
#endif
}

/** Drop comments and the contents of string and character literals, and every whitespace character, so
 *  the needles below match code only and whatever the formatting. */
std::string OhnhSquashCode(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    size_t i = 0;
    while (i < src.size()) {
        const char c = src[i];
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            const size_t eol = src.find('\n', i + 2);
            i = (eol == std::string::npos) ? src.size() : eol + 1;
            continue;
        }
        if (c == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            const size_t end = src.find("*/", i + 2);
            i = (end == std::string::npos) ? src.size() : end + 2;
            continue;
        }
        if (c == '"' || c == '\'') {
            out.push_back(c);
            i++;
            while (i < src.size() && src[i] != c) {
                i += (src[i] == '\\') ? 2 : 1;
            }
            if (i < src.size()) {
                out.push_back(c);
                i++;
            }
            continue;
        }
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            out.push_back(c);
        }
        i++;
    }
    return out;
}

/** The balanced `{...}` block that opens at `open` (inclusive), or empty. */
std::string OhnhBraceBlock(const std::string& code, size_t open) {
    if (open >= code.size() || code[open] != '{') {
        return std::string();
    }
    int depth = 0;
    for (size_t i = open; i < code.size(); i++) {
        if (code[i] == '{') {
            depth++;
        } else if (code[i] == '}') {
            depth--;
            if (depth == 0) {
                return code.substr(open, i - open + 1);
            }
        }
    }
    return std::string();
}

size_t OhnhCount(const std::string& hay, const std::string& needle) {
    size_t n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size())) {
        n++;
    }
    return n;
}

/** Empty when `WritePlaythrough`'s squashed body writes a location only behind the hidden guard; otherwise
 *  why not. */
const char* OhnhPlaythroughGuardProblem(const std::string& fileCode) {
    static const std::string kSig = "staticvoidWritePlaythrough(){";
    static const std::string kGuard = "if(!ctx->GetItemLocation(key)->IsHidden()){";
    static const std::string kWrite = "WriteLocation(sphereString,key,true);";
    const size_t sig = fileCode.find(kSig);
    if (sig == std::string::npos) {
        return "WritePlaythrough not found";
    }
    const std::string body = OhnhBraceBlock(fileCode, sig + kSig.size() - 1);
    if (body.empty()) {
        return "WritePlaythrough body is unbalanced";
    }
    const size_t guard = body.find(kGuard);
    if (guard == std::string::npos) {
        return "WritePlaythrough has no !IsHidden() guard";
    }
    const std::string guarded = OhnhBraceBlock(body, guard + kGuard.size() - 1);
    if (OhnhCount(guarded, kWrite) != 1) {
        return "the !IsHidden() guard does not hold the sphere's WriteLocation call";
    }
    if (OhnhCount(body, kWrite) != 1 || OhnhCount(body, "WriteLocation(") != 1) {
        return "WritePlaythrough writes a location outside the !IsHidden() guard";
    }
    return nullptr;
}

/** The matcher must accept upstream's body and reject a body that drops or bypasses the guard. */
const char* OhnhMatcherSelfCheck(void) {
    const std::string head = "static void WritePlaythrough() {\n    for (int i = 0; i < 3; ++i) {\n";
    const std::string good = head + "        if (!ctx->GetItemLocation(key)->IsHidden()) {\n"
                                    "            WriteLocation(sphereString, key, true);\n        }\n    }\n}\n";
    if (OhnhPlaythroughGuardProblem(OhnhSquashCode(good)) != nullptr) {
        return "rejected upstream's guarded body";
    }
    const std::string bad[] = {
        head + "        WriteLocation(sphereString, key, true);\n    }\n}\n",
        head + "        // if (!ctx->GetItemLocation(key)->IsHidden()) {\n"
               "        WriteLocation(sphereString, key, true);\n    }\n}\n",
        head + "        if (!ctx->GetItemLocation(key)->IsHidden()) {\n        }\n"
               "        WriteLocation(sphereString, key, true);\n    }\n}\n",
        head + "        if (!ctx->GetItemLocation(key)->IsHidden()) {\n"
               "            WriteLocation(sphereString, key, true);\n        }\n"
               "        WriteLocation(sphereString, key, false);\n    }\n}\n",
    };
    for (const std::string& body : bad) {
        if (OhnhPlaythroughGuardProblem(OhnhSquashCode(body)) == nullptr) {
            return "accepted a body whose WriteLocation is unguarded, commented out or doubled";
        }
    }
    return nullptr;
}

} // namespace

TestResult Test_OoTHiddenLocationNotHintable(void) {
    struct OhnhCase {
        int hintable;
        int hidden;
        int want;
    };
    static const OhnhCase kCases[] = {
        { 1, 0, 1 },
        { 1, 1, 0 },
        { 0, 0, 0 },
        { 0, 1, 0 },
    };
    for (const OhnhCase& c : kCases) {
        const int got = OoT_ItemLocation_TestHintableAnswer(c.hintable, c.hidden);
        if (got != c.want) {
            printf("[TEST] FAIL (oot-hidden-location-not-hintable): IsHintable() for hintable=%d hidden=%d "
                   "answered %d, want %d\n",
                   c.hintable, c.hidden, got, c.want);
            return TEST_FAIL;
        }
    }

#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL (oot-hidden-location-not-hintable): RSBS_SOURCE_DIR is undefined\n");
    return TEST_FAIL;
#else
    const char* why = OhnhMatcherSelfCheck();
    if (why != nullptr) {
        printf("[TEST] FAIL (oot-hidden-location-not-hintable): matcher self-check: %s\n", why);
        return TEST_FAIL;
    }
    const std::string src = OhnhReadSource("games/oot/soh/Enhancements/randomizer/3drando/spoiler_log.cpp");
    if (src.empty()) {
        printf("[TEST] FAIL (oot-hidden-location-not-hintable): cannot read 3drando/spoiler_log.cpp\n");
        return TEST_FAIL;
    }
    why = OhnhPlaythroughGuardProblem(OhnhSquashCode(src));
    if (why != nullptr) {
        printf("[TEST] FAIL (oot-hidden-location-not-hintable): %s\n", why);
        return TEST_FAIL;
    }
    printf("[TEST] PASS: oot-hidden-location-not-hintable\n");
    return TEST_PASS;
#endif
}
