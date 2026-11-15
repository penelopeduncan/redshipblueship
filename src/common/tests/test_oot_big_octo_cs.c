/**
 * @file test_oot_big_octo_cs.c
 * @brief ROM-free source regression check for the SoH #7158 backport (`oot-big-octo-onepoint-kept`).
 *
 * Included at file scope by `src/common/test_runner.cpp` (compiled as C++).
 *
 * Upstream SoH #7158 (`e2a82213c`) stops Skip One Point Cutscenes from skipping the Jabu-Jabu Big Octo
 * platform's one-point cutscene (BgBdanObjects params 0): the player was handed control while the platform was
 * still moving. In RSBS the skip lives in `TimeSaverOnVanillaBehaviorHandler`
 * (games/oot/soh/Enhancements/timesaver_hook_handlers.cpp), whose ACTORCAT_BG arm must break out before the
 * success chime and `*should = false` for that actor. Because that cutscene now always plays, the child Ruto skip
 * (SkipChildRutoInteractions.cpp) must keep camera setting 2 whatever the one-point CVar says, or the Ruto lift
 * cutscene queues twice.
 *
 * Matching runs on the source with comments removed and every whitespace character dropped, so formatting does
 * not matter but a commented-out or reordered guard does.
 */

#include "../test_runner.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string OboReadSource(const char* relPath) {
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

/** Copy @p src without comments and without any whitespace; string and char literals are kept verbatim. */
std::string OboCompactCode(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    for (size_t pos = 0; pos < src.size(); pos++) {
        const char c = src[pos];
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
            const size_t eol = src.find('\n', pos + 2);
            if (eol == std::string::npos) {
                break;
            }
            pos = eol;
            continue;
        }
        if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '*') {
            const size_t end = src.find("*/", pos + 2);
            if (end == std::string::npos) {
                break;
            }
            pos = end + 1;
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = c;
            out.push_back(quote);
            pos++;
            while (pos < src.size() && src[pos] != quote) {
                if (src[pos] == '\\' && pos + 1 < src.size()) {
                    out.push_back(src[pos]);
                    pos++;
                }
                out.push_back(src[pos]);
                pos++;
            }
            if (pos < src.size()) {
                out.push_back(quote);
            }
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        }
        out.push_back(c);
    }
    return out;
}

/** The compacted text from @p startNeedle up to (not including) @p endNeedle, or empty when either is missing. */
std::string OboSlice(const std::string& code, const char* startNeedle, const char* endNeedle) {
    const size_t start = code.find(startNeedle);
    if (start == std::string::npos) {
        return std::string();
    }
    const size_t end = code.find(endNeedle, start);
    if (end == std::string::npos) {
        return std::string();
    }
    return code.substr(start, end - start);
}

} // namespace

TestResult Test_OoTBigOctoOnePointKept(void) {
#ifndef RSBS_SOURCE_DIR
    printf("[TEST] FAIL (oot-big-octo-onepoint-kept): RSBS_SOURCE_DIR is undefined\n");
    return TEST_FAIL;
#else
    // Matcher self-check: comments vanish, literals survive, whitespace is dropped.
    if (OboCompactCode("a // x\n  b /* y */ c \"d e\"") != "abc\"d e\"") {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): matcher self-check\n");
        return TEST_FAIL;
    }

    // Leg 1: the one-point skip leaves the Big Octo platform's cutscene alone.
    const std::string handlers =
        OboCompactCode(OboReadSource("games/oot/soh/Enhancements/timesaver_hook_handlers.cpp"));
    if (handlers.empty()) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): cannot read timesaver_hook_handlers.cpp\n");
        return TEST_FAIL;
    }
    const std::string actorCase =
        OboSlice(handlers, "caseVB_PLAY_ONEPOINT_ACTOR_CS:", "caseVB_FREEZE_LINK_FOR_FOREST_PILLARS:");
    if (actorCase.empty()) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): VB_PLAY_ONEPOINT_ACTOR_CS case not found\n");
        return TEST_FAIL;
    }
    const std::string bgArm = OboSlice(actorCase, "caseACTORCAT_BG:", "switch(actor->id)");
    if (bgArm.empty()) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): ACTORCAT_BG arm not found\n");
        return TEST_FAIL;
    }
    static const char kBigOctoGuard[] = "if(actor->id==ACTOR_BG_BDAN_OBJECTS&&actor->params==0){break;}"
                                        "RateLimitedSuccessChime();*should=false;break;";
    if (bgArm.find(kBigOctoGuard) == std::string::npos) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): ACTORCAT_BG arm does not break out for "
               "BgBdanObjects params 0 before the chime and *should = false\n");
        return TEST_FAIL;
    }

    // Leg 2: the child Ruto skip keeps camera setting 2 regardless of the one-point CVar.
    const std::string ruto = OboCompactCode(
        OboReadSource("games/oot/soh/Enhancements/TimeSavers/SkipMiscInteractions/SkipChildRutoInteractions.cpp"));
    if (ruto.empty()) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): cannot read SkipChildRutoInteractions.cpp\n");
        return TEST_FAIL;
    }
    static const char kRutoCamera[] = "enRu1->action=42;";
    const std::string rutoArm = OboSlice(ruto, kRutoCamera, "func_80AEB124(");
    if (rutoArm.empty()) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): Ruto skip arm not found\n");
        return TEST_FAIL;
    }
    if (rutoArm.find("if(enRu1->unk_28C->cameraSetting!=2){enRu1->unk_28C->cameraSetting=1;}") == std::string::npos) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): Ruto skip does not guard cameraSetting != 2 alone\n");
        return TEST_FAIL;
    }
    if (rutoArm.find("TimeSavers.SkipCutscene.OnePoint") != std::string::npos) {
        printf("[TEST] FAIL (oot-big-octo-onepoint-kept): Ruto skip still reads the one-point CVar\n");
        return TEST_FAIL;
    }

    printf("[TEST] PASS: oot-big-octo-onepoint-kept\n");
    return TEST_PASS;
#endif
}
